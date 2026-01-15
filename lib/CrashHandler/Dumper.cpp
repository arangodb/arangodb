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

#include <format>
#include <queue>

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include "Basics/FileUtils.h"
#include "Basics/NumberOfCores.h"
#include "Basics/PhysicalMemory.h"
#include "Basics/files.h"
#include "CrashHandler/DataSource.h"
#include "Rest/Version.h"

namespace arangodb::crash_handler {

Dumper::Dumper(std::shared_ptr<DataSourceRegistry> dataSourceRegistry)
    : _dataSourceRegistry(std::move(dataSourceRegistry)) {}

void Dumper::setCrashesDirectory(std::string const& crashesDirectory) {
  _crashesDirectory = crashesDirectory;
  ensureCrashesDirectory();
}

void Dumper::createCrashDirectory() {
  auto const uuid = to_string(boost::uuids::random_generator()());
  auto const crashDirectory =
      arangodb::basics::FileUtils::buildFilename(*_crashesDirectory, uuid);
  arangodb::basics::FileUtils::createDirectory(crashDirectory);
}

void Dumper::dumpDataSources() {
  if (_crashesDirectory.has_value()) {
    return;
  }

  for (auto const* dataSource : _dataSourceRegistry->getDataSources()) {
    std::string filename = arangodb::basics::FileUtils::buildFilename(
        *_crashesDirectory,
        std::format("{}.json", dataSource->getDataSourceName()));
    auto data = dataSource->getCrashData();
    arangodb::basics::FileUtils::spit(filename, data.toJson());
  }
}

void Dumper::dumpSystemInfo() {
  if (!_crashesDirectory.has_value()) {
    return;
  }

  auto const sysInfoFilename = arangodb::basics::FileUtils::buildFilename(
      *_crashesDirectory, "system_info.txt");

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

  arangodb::basics::FileUtils::spit(sysInfoFilename, sysInfo);
}

void Dumper::dumpBacktractInfo(std::string_view backtrace) {
  if (backtrace.empty() || !_crashesDirectory.has_value()) {
    return;
  }
  auto const backtraceFilename = arangodb::basics::FileUtils::buildFilename(
      *_crashesDirectory, "backtrace.txt");
  arangodb::basics::FileUtils::spit(backtraceFilename, backtrace.data(),
                                    backtrace.size());
}

void Dumper::ensureCrashesDirectory() {
  if (!_crashesDirectory.has_value()) {
    return;
  }

  if (!arangodb::basics::FileUtils::exists(*_crashesDirectory)) {
    arangodb::basics::FileUtils::createDirectory(*_crashesDirectory);
  }
}

void Dumper::cleanupOldCrashDirectories(std::string const& crashesDirectory,
                                        size_t maxCrashDirectories) {
  if (crashesDirectory.empty() ||
      !arangodb::basics::FileUtils::isDirectory(crashesDirectory)) {
    return;
  }

  auto const entries = arangodb::basics::FileUtils::listFiles(crashesDirectory);
  std::priority_queue<std::pair<int64_t, std::string>,
                      std::vector<std::pair<int64_t, std::string>>,
                      std::greater<>>
      crashDirs;

  for (auto const& entry : entries) {
    auto const fullPath =
        arangodb::basics::FileUtils::buildFilename(crashesDirectory, entry);
    if (arangodb::basics::FileUtils::isDirectory(fullPath)) {
      int64_t mtime{0};
      if (TRI_MTimeFile(fullPath.c_str(), &mtime) == TRI_ERROR_NO_ERROR) {
        crashDirs.emplace(mtime, entry);
      }
    }
  }

  // Remove the oldest directories until we're at the limit
  while (crashDirs.size() > maxCrashDirectories) {
    auto const& [mtime, dirName] = crashDirs.top();
    auto const crashDir =
        arangodb::basics::FileUtils::buildFilename(crashesDirectory, dirName);
    TRI_RemoveDirectory(crashDir.c_str());
    crashDirs.pop();
  }
}

}  // namespace arangodb::crash_handler
