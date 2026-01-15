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

#include <algorithm>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace arangodb::crash_handler {

class CrashHandlerDataSource;

/// @brief Interface for crash handler operations that can be accessed
/// through the CrashHandlerFeature. This allows decoupling the feature
/// from the concrete CrashHandler implementation.
class CrashHandlerRegistry {
 public:
  virtual ~CrashHandlerRegistry() = default;

  /// @brief sets the database directory for crash dumps
  virtual void setDatabaseDirectory(std::string path) = 0;

  /// @brief lists all crash directories (returns UUIDs)
  virtual std::vector<std::string> listCrashes() = 0;

  /// @brief gets the contents of a specific crash directory
  /// Returns a map of filename -> file contents
  virtual std::unordered_map<std::string, std::string> getCrashContents(
      std::string_view crashId) = 0;

  /// @brief deletes a specific crash directory
  /// Returns true if successful, false if not found
  virtual bool deleteCrash(std::string_view crashId) = 0;

  void addCrashHandlerDataSource(CrashHandlerDataSource const* dataSource) {
    std::lock_guard guard(_dataSourceMtx);
    _dataSources.push_back(dataSource);
  }

  std::vector<CrashHandlerDataSource const*> const& getCrashHandlerDataSources()
      const {
    return _dataSources;
  }

  void removeCrashHandlerDataSource(CrashHandlerDataSource const* dataSource) {
    std::lock_guard guard(_dataSourceMtx);
    std::ranges::remove(_dataSources, dataSource);
  }

 private:
  std::mutex _dataSourceMtx;
  std::vector<CrashHandlerDataSource const*> _dataSources;
};

}  // namespace arangodb::crash_handler
