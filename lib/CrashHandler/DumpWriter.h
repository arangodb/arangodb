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

#include <filesystem>
#include <memory>
#include <string_view>

#include "CrashHandler/DataSourceRegistry.h"

namespace arangodb::crash_handler {

/// Dumps the data of crash handler to the disk
class DumpWriter {
 public:
  DumpWriter(std::filesystem::path crashDirectory,
             std::shared_ptr<DataSourceRegistry> dataSourceRegistry);

  void dumpData(std::string_view backtrace) const;

 private:
  void dumpDataSources() const;

  void dumpSystemInfo() const;

  void dumpBacktraceInfo(std::string_view backtrace) const;

  std::filesystem::path _crashDirectory;

  std::shared_ptr<DataSourceRegistry> _dataSourceRegistry;
};

}  // namespace arangodb::crash_handler
