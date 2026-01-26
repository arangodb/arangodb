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

#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

#include "CrashHandler/DataSourceRegistry.h"

namespace arangodb::crash_handler {

static constexpr size_t kMaxCrashDirectories{10};

/// Manages the crash dumps on disk
class DumpManager {
 public:
  DumpManager(std::shared_ptr<DataSourceRegistry> dataSourceRegistry);

  /// @brief lists all crash directories (returns UUIDs)
  std::vector<std::string> listCrashes() const;

  /// @brief gets the contents of a specific crash directory
  std::unordered_map<std::string, std::string> getCrashContents(
      std::string_view crashId) const;

  /// @brief deletes a specific crash directory
  bool deleteCrash(std::string_view crashId);

  void setCrashesDirectory(std::filesystem::path const& crashesDirectory);

  void removeOldCrashDirectories(size_t maxCrashDirectories) const;

  /// @brief dumps crash data (data sources, system info, backtrace)
  void dumpCrashData(std::string_view backtrace) const;

 private:
  std::filesystem::path _crashesDirectory;
  std::shared_ptr<DataSourceRegistry> _dataSourceRegistry;
};

}  // namespace arangodb::crash_handler
