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

/// @brief translate to fuerte's ApiVersion. The switch is exhaustive on
/// purpose: adding an enumerator to arangodb::ApiVersion without giving it a
/// fuerte counterpart is a compile error (-Wswitch), which is what keeps the
/// two definitions in lockstep.
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

// The mapping above keeps the enumerator *sets* in lockstep; these keep the
// numeric values in lockstep, which matters wherever either enum is
// serialized as a number.
static_assert(toNumber(ApiVersion::V0) ==
              static_cast<unsigned>(fuerte::api_version::ApiVersion::V0));
static_assert(toNumber(ApiVersion::V1) ==
              static_cast<unsigned>(fuerte::api_version::ApiVersion::V1));
static_assert(
    toNumber(ApiVersion::Experimental) ==
    static_cast<unsigned>(fuerte::api_version::ApiVersion::Experimental));

}  // namespace arangodb
