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

#include "CrashHandler/DumpManager.h"

#include <filesystem>
#include <queue>

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include "Basics/FileUtils.h"
#include "Basics/files.h"

namespace arangodb::crash_handler {

DumpManager::DumpManager(std::shared_ptr<DataSourceRegistry> dataSourceRegistry)
    : _dataSourceRegistry(std::move(dataSourceRegistry)) {}

void DumpManager::setCrashesDirectory(std::string const& crashesDirectory) {
  _crashesDirectory =
      (std::filesystem::path(crashesDirectory) / "crashes").string();
  if (!std::filesystem::exists(_crashesDirectory)) {
    std::filesystem::create_directories(_crashesDirectory);
  }
}

std::vector<std::string> DumpManager::listCrashes() const {
  std::vector<std::string> crashes;
  if (_crashesDirectory.empty()) {
    return {};
  }

  auto entries = basics::FileUtils::listFiles(_crashesDirectory);
  for (auto const& entry : entries) {
    auto fullPath = basics::FileUtils::buildFilename(_crashesDirectory, entry);
    if (basics::FileUtils::isDirectory(fullPath)) {
      crashes.push_back(entry);
    }
  }
  return crashes;
}

std::unordered_map<std::string, std::string> DumpManager::getCrashContents(
    std::string_view crashId) const {
  if (_crashesDirectory.empty()) {
    return {};
  }

  std::unordered_map<std::string, std::string> contents;
  auto crashDir =
      basics::FileUtils::buildFilename(_crashesDirectory, std::string(crashId));
  if (!basics::FileUtils::isDirectory(crashDir)) {
    return contents;
  }
  auto const files = basics::FileUtils::listFiles(crashDir);
  for (auto const& file : files) {
    auto const filePath = basics::FileUtils::buildFilename(crashDir, file);
    if (basics::FileUtils::isRegularFile(filePath)) {
      try {
        contents[file] = basics::FileUtils::slurp(filePath);
      } catch (...) {
        // Skip files that can't be read
      }
    }
  }
  return contents;
}

bool DumpManager::deleteCrash(std::string_view crashId) {
  if (_crashesDirectory.empty()) {
    return false;
  }
  auto crashDir = arangodb::basics::FileUtils::buildFilename(
      _crashesDirectory, std::string(crashId));
  if (!arangodb::basics::FileUtils::isDirectory(crashDir)) {
    return false;
  }
  auto res = TRI_RemoveDirectory(crashDir.c_str());
  return res == TRI_ERROR_NO_ERROR;
}

void DumpManager::cleanupOldCrashDirectories(size_t maxCrashDirectories) const {
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

DumpWriter DumpManager::getDumpWriter() const {
  return DumpWriter(_crashesDirectory, _dataSourceRegistry);
}

}  // namespace arangodb::crash_handler
