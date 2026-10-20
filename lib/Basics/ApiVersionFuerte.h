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

// The adapter between the server's ApiVersion and fuerte's own definition.
// Only code that actually talks to fuerte should include this header - plain
// ApiVersion.h deliberately stays free of any fuerte dependency.

#include "Basics/ApiVersion.h"

#include "fuerte/ApiVersion.h"

namespace arangodb {

// The two enums are kept in lockstep by three checks:
// - toFuerte and fromFuerte switch exhaustively over one enum each, so an
//   enumerator added on either side without a counterpart is a compile error
//   (-Wswitch, which fails the build only with -Werror).
// - The round trips below catch a wrong mapping in either direction.
// - The value checks below fail unconditionally if an enumerator is inserted
//   or reordered on either side.

/// @brief translate to fuerte's ApiVersion
constexpr fuerte::api_version::ApiVersion toFuerte(ApiVersion version) {
  switch (version) {
    case ApiVersion::V0:
      return fuerte::api_version::ApiVersion::V0;
    case ApiVersion::V1:
      return fuerte::api_version::ApiVersion::V1;
    case ApiVersion::Experimental:
      return fuerte::api_version::ApiVersion::Experimental;
  }
  return fuerte::api_version::ApiVersion::V0;
}

/// @brief translate from fuerte's ApiVersion
constexpr ApiVersion fromFuerte(fuerte::api_version::ApiVersion version) {
  switch (version) {
    case fuerte::api_version::ApiVersion::V0:
      return ApiVersion::V0;
    case fuerte::api_version::ApiVersion::V1:
      return ApiVersion::V1;
    case fuerte::api_version::ApiVersion::Experimental:
      return ApiVersion::Experimental;
  }
  return ApiVersion::V0;
}

static_assert(fromFuerte(toFuerte(ApiVersion::V0)) == ApiVersion::V0);
static_assert(fromFuerte(toFuerte(ApiVersion::V1)) == ApiVersion::V1);
static_assert(fromFuerte(toFuerte(ApiVersion::Experimental)) ==
              ApiVersion::Experimental);
static_assert(toFuerte(fromFuerte(fuerte::api_version::ApiVersion::V0)) ==
              fuerte::api_version::ApiVersion::V0);
static_assert(toFuerte(fromFuerte(fuerte::api_version::ApiVersion::V1)) ==
              fuerte::api_version::ApiVersion::V1);
static_assert(
    toFuerte(fromFuerte(fuerte::api_version::ApiVersion::Experimental)) ==
    fuerte::api_version::ApiVersion::Experimental);

static_assert(static_cast<unsigned>(ApiVersion::V0) ==
              static_cast<unsigned>(fuerte::api_version::ApiVersion::V0));
static_assert(static_cast<unsigned>(ApiVersion::V1) ==
              static_cast<unsigned>(fuerte::api_version::ApiVersion::V1));
static_assert(
    static_cast<unsigned>(ApiVersion::Experimental) ==
    static_cast<unsigned>(fuerte::api_version::ApiVersion::Experimental));

}  // namespace arangodb
