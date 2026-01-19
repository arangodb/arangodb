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
/// @author Jure Bajic
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace arangodb::crash_handler {

class ICrashRegistry {
 public:
  virtual ~ICrashRegistry() = default;

  /// @brief lists all crash directories (returns UUIDs)
  virtual std::vector<std::string> listCrashes() const = 0;

  /// @brief gets the contents of a specific crash directory
  /// Returns a map of filename -> file contents
  virtual std::unordered_map<std::string, std::string> getCrashContents(
      std::string_view crashId) const = 0;

  /// @brief deletes a specific crash directory
  /// Returns true if successful, false if not found
  virtual bool deleteCrash(std::string_view crashId) = 0;
};

}  // namespace arangodb::crash_handler
