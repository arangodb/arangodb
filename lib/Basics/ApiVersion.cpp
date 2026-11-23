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

#include "Basics/ApiVersion.h"

#include "Basics/ApiVersionFuerte.h"  // included to make sure both Api versions agree with each other
#include "Basics/Result.h"
#include "Basics/StaticStrings.h"
#include "Basics/voc-errors.h"

#include <absl/strings/str_cat.h>

#include <charconv>
#include <optional>
#include <ostream>

#ifndef ARANGODB_VERSION_MAJOR
#error \
    "ARANGODB_VERSION_MAJOR must be defined by the build (lib/CMakeLists.txt)"
#endif

namespace arangodb {

namespace {

// the branch tag always matches the major product version
constexpr unsigned branchTag = ARANGODB_VERSION_MAJOR;

struct VersionNumber {
  unsigned major;
  unsigned minor;
  unsigned branch = branchTag;
};

// Each major API version is served with exactly one minor version. This is
// the only place where minor versions are maintained.
constexpr unsigned v1Minor = 0;

constexpr std::optional<VersionNumber> servedVersion(ApiVersion version) {
  switch (version) {
    case ApiVersion::V1:
      return VersionNumber{.major = 1, .minor = v1Minor};
    case ApiVersion::Experimental:
      return std::nullopt;
  }
  return std::nullopt;
}

// Parse decimal number without sign or leading zeros.
std::optional<unsigned> parseNumber(std::string_view s) {
  if (s.empty() || (s.size() > 1 && s.front() == '0')) {
    return std::nullopt;
  }
  unsigned value{};
  auto const [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
  if (ec != std::errc{} || ptr != s.data() + s.size()) {
    return std::nullopt;
  }
  return value;
}

// Parse input strings of the form vMAJOR.MINOR-BRANCH.
std::optional<VersionNumber> parseVersionNumber(std::string_view s) {
  if (!s.starts_with('v')) {
    return std::nullopt;
  }
  auto const dot = s.find('.');
  auto const dash = s.find('-', dot);
  if (dot == std::string_view::npos || dash == std::string_view::npos) {
    return std::nullopt;
  }
  auto const major = parseNumber(s.substr(1, dot - 1));
  auto const minor = parseNumber(s.substr(dot + 1, dash - dot - 1));
  auto const branch = parseNumber(s.substr(dash + 1));
  if (!major || !minor || !branch) {
    return std::nullopt;
  }
  return VersionNumber{.major = *major, .minor = *minor, .branch = *branch};
}

std::string format(VersionNumber const& v) {
  return absl::StrCat("v", v.major, ".", v.minor, "-", v.branch);
}

}  // namespace

std::ostream& operator<<(std::ostream& os, ApiVersion version) {
  return os << to_string(version);
}

namespace api_version {

bool isApiVersionRequestable(ApiVersion version) {
  return isApiVersionSupported(version);
}

std::string formatServedApiVersion(ApiVersion version) {
  auto const served = servedVersion(version);
  return served ? format(*served) : std::string{to_string(version)};
}

Result checkRequestedApiVersion(ApiVersion version, std::string_view header) {
  auto const served = servedVersion(version);
  if (!served) {
    return {TRI_ERROR_BAD_PARAMETER,
            absl::StrCat("the ", StaticStrings::XArangoApiVersion,
                         " header must not be used with API version ",
                         to_string(version))};
  }
  auto const requested = parseVersionNumber(header);
  if (!requested) {
    return {
        TRI_ERROR_BAD_PARAMETER,
        absl::StrCat("invalid ", StaticStrings::XArangoApiVersion, " header '",
                     header, "', expected vMAJOR.MINOR-BRANCH")};
  }
  // A client may rely on everything up to the served version, never beyond.
  if (requested->major != served->major || requested->minor > served->minor ||
      requested->branch > served->branch) {
    return {TRI_ERROR_HTTP_API_VERSION_MISMATCH,
            absl::StrCat("requested API version ", header,
                         " is not compatible with served API version ",
                         format(*served))};
  }
  return {};
}

}  // namespace api_version
}  // namespace arangodb
