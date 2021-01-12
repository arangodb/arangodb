////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Manuel Baesler
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AUTHENTICATION_COMMON_H
#define ARANGOD_AUTHENTICATION_COMMON_H 1

#include "Basics/Common.h"
#include <velocypack/Slice.h>

namespace arangodb {
namespace auth {

/// Supported access levels for data
enum class Level : char { UNDEFINED = 0, NONE = 1, RO = 2, RW = 3 };

/// Supported source types of users sources
enum class Source : char { Local, LDAP };

auth::Level convertToAuthLevel(velocypack::Slice grants);
auth::Level convertToAuthLevel(std::string const& grant);
std::string convertFromAuthLevel(auth::Level lvl);

}  // namespace auth
}  // namespace arangodb

#endif
