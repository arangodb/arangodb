////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
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

#include "Basics/Result.h"
#include <velocypack/Slice.h>

#include <string_view>

namespace arangodb::auth {

/// Supported access levels for data
enum class Level : char { UNDEFINED = 0, NONE = 1, RO = 2, RW = 3 };

auth::Level convertToAuthLevel(velocypack::Slice grants);
auth::Level convertToAuthLevel(std::string_view grant);
std::string_view convertFromAuthLevel(auth::Level lvl);

inline Result isNameAndNoId(std::string_view name) {
  if (!name.empty() && (name[0] >= '0' && name[0] <= '9')) {
    return {TRI_ERROR_FORBIDDEN, "Name must not be an ID (numerical) here!"};
  }
  return {};
}

}  // namespace arangodb::auth
