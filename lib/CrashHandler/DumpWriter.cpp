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

#include "CrashHandler/DumpWriter.h"

#include <filesystem>
#include <format>
#include <fstream>

#include "Basics/NumberOfCores.h"
#include "Basics/PhysicalMemory.h"
#include "CrashHandler/DataSource.h"
#include "Rest/Version.h"

namespace arangodb::crash_handler {

DumpWriter::DumpWriter(std::filesystem::path crashDirectory,
                       std::shared_ptr<DataSourceRegistry> dataSourceRegistry)
    : _crashDirectory(std::move(crashDirectory)),
      _dataSourceRegistry(std::move(dataSourceRegistry)) {}

void DumpWriter::dumpData(std::string_view backtrace) const {
  dumpDataSources();
  dumpSystemInfo();
  if (!backtrace.empty()) {
    dumpBacktraceInfo(backtrace);
  }
}

void DumpWriter::dumpDataSources() const {
  for (auto const* dataSource : _dataSourceRegistry->getDataSources()) {
    auto const filename =
        _crashDirectory /
        std::format("{}.json", dataSource->getDataSourceName());
    auto data = dataSource->getCrashData();
    std::ofstream ofs(filename);
    ofs << data.toJson();
  }
}

void DumpWriter::dumpSystemInfo() const {
  auto const sysInfoFilename = _crashDirectory / "system_info.txt";

  std::string sysInfo = std::format(
      "ArangoDB Version: {}\n"
      "Platform: {}\n"
      "Number of Cores: {}\n"
      "Effective Number of Cores: {}\n"
      "Physical Memory: {} bytes\n"
      "Effective Physical Memory: {} bytes\n",
      arangodb::rest::Version::getServerVersion(),
      arangodb::rest::Version::getPlatform(),
      arangodb::NumberOfCores::getValue(),
      arangodb::NumberOfCores::getEffectiveValue(),
      arangodb::PhysicalMemory::getValue(),
      arangodb::PhysicalMemory::getEffectiveValue());

  std::ofstream ofs(sysInfoFilename);
  ofs << sysInfo;
}

void DumpWriter::dumpBacktraceInfo(std::string_view backtrace) const {
  auto const backtraceFilename = _crashDirectory / "backtrace.txt";
  std::ofstream ofs(backtraceFilename);
  ofs.write(backtrace.data(), static_cast<std::streamsize>(backtrace.size()));
}

}  // namespace arangodb::crash_handler
