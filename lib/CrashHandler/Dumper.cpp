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

#include "CrashHandler/Dumper.h"

#include <filesystem>
#include <format>
#include <fstream>
#include <queue>

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include "Basics/NumberOfCores.h"
#include "Basics/PhysicalMemory.h"
#include "CrashHandler/DataSource.h"
#include "Rest/Version.h"

namespace arangodb::crash_handler {

Dumper::Dumper(std::shared_ptr<DataSourceRegistry> dataSourceRegistry)
    : _dataSourceRegistry(std::move(dataSourceRegistry)) {}

void Dumper::setCrashesDirectory(std::string const& crashesDirectory) {
  _crashesDirectory = crashesDirectory;
}

std::string Dumper::createCrashDirectory() const {
  auto const uuid = to_string(boost::uuids::random_generator()());
  auto const crashDirectory = std::filesystem::path(_crashesDirectory) / uuid;
  std::filesystem::create_directory(crashDirectory);

  return crashDirectory.string();
}

void Dumper::dumpCrashData(std::string_view const backtrace) const {
  if (!ensureCrashesDirectory()) {
    return;
  }

  auto const crashDirectory = createCrashDirectory();
  dumpDataSources(crashDirectory);
  dumpSystemInfo(crashDirectory);
  if (!backtrace.empty()) {
    dumpBacktraceInfo(crashDirectory, backtrace);
  }
}

void Dumper::dumpDataSources(std::string const& crashDirectory) const {
  for (auto const* dataSource : _dataSourceRegistry->getDataSources()) {
    auto const filename =
        std::filesystem::path(crashDirectory) /
        std::format("{}.json", dataSource->getDataSourceName());
    auto data = dataSource->getCrashData();
    std::ofstream ofs(filename);
    ofs << data.toJson();
  }
}

void Dumper::dumpSystemInfo(std::string const& crashDirectory) const {
  auto const sysInfoFilename =
      std::filesystem::path(crashDirectory) / "system_info.txt";

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

void Dumper::dumpBacktraceInfo(std::string const& crashDirectory,
                               std::string_view const backtrace) const {
  auto const backtraceFilename =
      std::filesystem::path(crashDirectory) / "backtrace.txt";
  std::ofstream ofs(backtraceFilename);
  ofs.write(backtrace.data(), static_cast<std::streamsize>(backtrace.size()));
}

bool Dumper::ensureCrashesDirectory() const {
  if (_crashesDirectory.empty()) {
    return false;
  }

  if (!std::filesystem::exists(_crashesDirectory)) {
    std::filesystem::create_directories(_crashesDirectory);
  }

  return true;
}

void Dumper::cleanupOldCrashDirectories(size_t maxCrashDirectories) const {
  if (_crashesDirectory.empty() ||
      !std::filesystem::is_directory(_crashesDirectory)) {
    return;
  }

  std::priority_queue<
      std::pair<std::filesystem::file_time_type, std::filesystem::path>,
      std::vector<
          std::pair<std::filesystem::file_time_type, std::filesystem::path>>,
      std::greater<>>
      crashDirs;

  for (auto const& entry :
       std::filesystem::directory_iterator(_crashesDirectory)) {
    if (entry.is_directory()) {
      auto mtime = entry.last_write_time();
      crashDirs.emplace(mtime, entry.path());
    }
  }

  // Remove the oldest directories until we're at the limit
  while (crashDirs.size() > maxCrashDirectories) {
    auto const& [mtime, crashDir] = crashDirs.top();
    std::filesystem::remove_all(crashDir);
    crashDirs.pop();
  }
}

}  // namespace arangodb::crash_handler
