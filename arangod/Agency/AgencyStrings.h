////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_AGENCY_AGENCY_STRINGS_H
#define ARANGOD_AGENCY_AGENCY_STRINGS_H

#include <string>

namespace arangodb {
namespace consensus {



constexpr char const* ANALYZERS = "Analyzers";
constexpr char const* COLLECTIONS = "Collections";
constexpr char const* DATABASES = "Databases";
constexpr char const* VIEWS = "Views";
constexpr char const* RECONFIGURE = ".agency";
constexpr char const* VERSION = "Version";

constexpr char const* CURRENT = "Current";
constexpr char const* CURRENT_VERSION = "Current/Version";
constexpr char const* CURRENT_COLLECTIONS = "Current/Collections/";
constexpr char const* CURRENT_DATABASES = "Current/Databases/";

constexpr char const* PLAN = "Plan";
constexpr char const* PLAN_VERSION = "Plan/Version";
constexpr char const* PLAN_COLLECTIONS = "Plan/Collections/";
constexpr char const* PLAN_DATABASES = "Plan/Databases/";
constexpr char const* PLAN_VIEWS = "Plan/Views/";
constexpr char const* PLAN_ANALYZERS = "Plan/Analyzers/";

constexpr char const* HOTBACKUP_KEY = "/Target/HotBackup/Create";

constexpr char const* PREC_IS_READ_LOCKED = "is-read-locked";
constexpr char const* PREC_IS_WRITE_LOCKED = "is-write-locked";
constexpr char const* PREC_CAN_READ_LOCK = "can-read-lock";
constexpr char const* PREC_CAN_WRITE_LOCK = "can-write-lock";


constexpr char const* OP_READ_LOCK = "read-lock";
constexpr char const* OP_WRITE_LOCK = "write-lock";
constexpr char const* OP_READ_UNLOCK = "read-unlock";
constexpr char const* OP_WRITE_UNLOCK = "write-unlock";

constexpr char const* OP_CMP = "cmp";

}  // namespace consensus
}  // namespace arangodb

#endif
