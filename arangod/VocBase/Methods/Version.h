////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_VOC_BASE_METHODS_VERSION_H
#define ARANGOD_VOC_BASE_METHODS_VERSION_H 1

#include "Basics/Result.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <cstdint>
#include <map>

struct TRI_vocbase_t;

namespace arangodb {
namespace methods {

/// not based on Basics/Result because I did not
/// feel like adding these error codes globally.
/// Originally from js/server/database-version.js
struct VersionResult {
  enum StatusCode : int {
    INVALID = 0,
    VERSION_MATCH = 1,
    UPGRADE_NEEDED = 2,
    DOWNGRADE_NEEDED = 3,
    CANNOT_PARSE_VERSION_FILE = -2,
    CANNOT_READ_VERSION_FILE = -3,
    NO_VERSION_FILE = -4,
    NO_SERVER_VERSION = -5
  };

  /// @brief status code
  StatusCode status;
  /// @brief current server version
  uint64_t serverVersion;
  /// @brief version in VERSION file on disk
  uint64_t databaseVersion;
  /// @brief tasks that were executed
  std::map<std::string, bool> tasks;
};

/// Code to create and initialize databases
/// Replaces upgrade-database.js for good
struct Version {
  /// @brief "(((major * 100) + minor) * 100) + patch"
  static uint64_t current();
  /// @brief read the VERSION file for a database
  static VersionResult check(TRI_vocbase_t*);
  static VersionResult::StatusCode compare(uint64_t current, uint64_t other);
  /// @brief write a VERSION file including all tasks
  static Result write(TRI_vocbase_t*, std::map<std::string, bool> const& tasks, bool sync);

  static uint64_t parseVersion(const char* str);

 private:
  static uint64_t parseVersion(const char* str, size_t len);
};
}  // namespace methods
}  // namespace arangodb

#endif
