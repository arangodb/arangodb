////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2025 ArangoDB GmbH, Cologne, Germany
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

#include "RestServer/arangod.h"

namespace arangodb {

class CrashHandlerRegistry;

/// @brief Feature to control crash dump logging to the database directory.
/// The CrashHandler itself always runs for crash handling, but this feature
/// controls whether additional crash information is written to disk.
class CrashHandlerFeature final : public ArangodFeature {
 public:
  static constexpr std::string_view name() noexcept { return "CrashHandler"; }

  explicit CrashHandlerFeature(Server& server,
                               CrashHandlerRegistry* crashHandler);

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;

  /// @brief returns true if crash dump logging is enabled
  bool isEnabled() const noexcept { return _enabled; }

  /// @brief returns the crash handler interface pointer
  CrashHandlerRegistry* crashHandler() const noexcept { return _crashHandler; }

  /// @brief sets the database directory for crash dumps
  void setDatabaseDirectory(std::string path);

  /// @brief lists all crash directories (returns UUIDs)
  std::vector<std::string> listCrashes() const;

  /// @brief gets the contents of a specific crash directory
  std::unordered_map<std::string, std::string> getCrashContents(
      std::string_view crashId) const;

  /// @brief deletes a specific crash directory
  bool deleteCrash(std::string_view crashId) const;

 private:
  /// @brief pointer to the CrashHandler interface (not owned)
  /// This is so we don't directly link with CrashHandler
  /// which breaks test dependencies
  CrashHandlerRegistry* _crashHandler;

  /// @brief whether crash dump logging is enabled
  bool _enabled;
};

}  // namespace arangodb
