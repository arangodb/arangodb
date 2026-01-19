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
#include <string>
#include <string_view>
#include <memory>

#include "CrashHandler/DataSourceRegistry.h"

namespace arangodb::crash_handler {

/// Dumps the data of crash handler to the disk
class Dumper {
 public:
  Dumper(std::shared_ptr<DataSourceRegistry> dataSourceRegistry);

  void setCrashesDirectory(std::string const& crashesDirectory);

  void cleanupOldCrashDirectories(size_t maxCrashDirectories) const;

  void dumpCrashData(std::string_view backtrace) const;

 private:
  // Create a new crash directory for the current crash
  std::string createCrashDirectory() const;

  void dumpDataSources(std::string const& crashDirectory) const;

  void dumpSystemInfo(std::string const& crashDirectory) const;

  void dumpBacktraceInfo(std::string const& crashDirectory,
                         std::string_view const backtrace) const;

  // Ensure that the crashes directory exists
  bool ensureCrashesDirectory() const;

  std::string _crashesDirectory;

  std::shared_ptr<DataSourceRegistry> _dataSourceRegistry;
};

}  // namespace arangodb::crash_handler
