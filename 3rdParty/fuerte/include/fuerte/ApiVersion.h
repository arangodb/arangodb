////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
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
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace arangodb { namespace fuerte { namespace api_version {
inline namespace v1 {

/// ApiVersion is also used by ArangoDB server but is defined here (in fuerte)
/// so that fuerte does not need to include server headers.
/// lib/Rest/ApiVersion.h re-exports this enum.

enum class ApiVersion { V0 = 0, V1, Experimental };

constexpr auto from(std::string_view s) -> std::optional<ApiVersion> {
  if (s == "v0") {
    return ApiVersion::V0;
  } else if (s == "v1") {
    return ApiVersion::V1;
  } else if (s == "experimental") {
    return ApiVersion::Experimental;
  }
  return std::nullopt;
}

constexpr auto to_string(ApiVersion v) -> std::string {
  switch (v) {
    case ApiVersion::V0:
      return "v0";
    case ApiVersion::V1:
      return "v1";
    case ApiVersion::Experimental:
      return "experimental";
  }
}

// the inspector only works when we link with the inspector library
template <typename Inspector>
auto inspect(Inspector& f, ApiVersion& x) {
  if constexpr (Inspector::isLoading) {
    auto str = std::string{};
    auto res = f.apply(str);
    if (!res.ok()) {
      return res;
    }
    if (auto opt = from(str); opt.has_value()) {
      x = *opt;
    }
    return decltype(res){"invalid ApiVersion: " + str};
  } else {
    return f.apply(to_string(x));
  }
}

}}}}  // namespace arangodb::fuerte::api_version::v1
