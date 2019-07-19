////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Kaveh Vahedipour
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AGENCY_STRINGS_H
#define ARANGOD_AGENCY_STRINGS_H 1

#include <string>
#include "Basics/StringUtils.h"

namespace arangodb {
namespace consensus {

constexpr char const* ARANGO = "arango";

constexpr char const* DATABASES = "Databases";
constexpr char const* COLLECTIONS = "Collections";
constexpr char const* RECONFIGURE = ".agency";
constexpr char const* VERSION = "Version";

constexpr char const* CURRENT = "Current";
constexpr char const* CURRENT_VERSION = "Current/Version";
constexpr char const* CURRENT_COLLECTIONS = "Current/Collections/";
constexpr char const* CURRENT_DATABASES = "Current/Databases/";

constexpr char const* SERVERS_REGISTERED =  "ServersRegistered";

constexpr char const* PLAN = "Plan";
constexpr char const* PLAN_VERSION = "Plan/Version";
constexpr char const* PLAN_COLLECTIONS = "Plan/Collections/";
constexpr char const* PLAN_DATABASES = "Plan/Databases/";

constexpr char const* TARGET = "Target";
constexpr char const* MAP_UNIQUE_TO_SHORT_ID = "MapUniqueToShortId";

////////////////////////////////////////////////////////////////////////////////
/// @brief helper function to conveniently assemble paths from string constants
///
///        this should really be moved to a more appropriate place, and maybe
///        do checks and/or trims on the strings that are passed in.
////////////////////////////////////////////////////////////////////////////////
static inline std::string concatPath(std::vector<std::string> const& components) {
  return arangodb::basics::StringUtils::join(components, "/");
}

}  // namespace consensus
}  // namespace arangodb

#endif
