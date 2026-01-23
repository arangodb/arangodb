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
#include <fstream>
#include <sstream>

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include "CrashHandler/DumpWriter.h"

namespace arangodb::crash_handler {

DumpManager::DumpManager(std::shared_ptr<DataSourceRegistry> dataSourceRegistry)
    : _dataSourceRegistry(std::move(dataSourceRegistry)) {}

void DumpManager::setCrashesDirectory(
    std::filesystem::path const& crashesDirectory) {
  _crashesDirectory = crashesDirectory / "crashes";
  if (!std::filesystem::exists(_crashesDirectory)) {
    std::filesystem::create_directories(_crashesDirectory);
  }

  removeOldCrashDirectories(kMaxCrashDirectories);
}

std::vector<std::string> DumpManager::listCrashes() const {
  std::vector<std::string> crashes;
  if (_crashesDirectory.empty() ||
      !std::filesystem::is_directory(_crashesDirectory)) {
    return {};
  }

  for (auto const& entry :
       std::filesystem::directory_iterator(_crashesDirectory)) {
    if (entry.is_directory()) {
      crashes.push_back(entry.path().filename().string());
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
  auto const crashDir = _crashesDirectory / std::string(crashId);
  if (!std::filesystem::is_directory(crashDir)) {
    return contents;
  }
  for (auto const& entry : std::filesystem::directory_iterator(crashDir)) {
    if (entry.is_regular_file()) {
      try {
        std::ifstream file(entry.path(), std::ios::binary);
        if (file) {
          std::ostringstream ss;
          ss << file.rdbuf();
          contents[entry.path().filename().string()] = ss.str();
        }
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
  auto const crashDir = _crashesDirectory / std::string(crashId);
  if (!std::filesystem::is_directory(crashDir)) {
    return false;
  }
  std::error_code ec;
  std::filesystem::remove_all(crashDir, ec);
  return !ec;
}

void DumpManager::removeOldCrashDirectories(size_t maxCrashDirectories) const {
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

void DumpManager::dumpCrashData(std::string_view backtrace) const {
  if (_crashesDirectory.empty()) {
    return;
  }

  auto const uuid = to_string(boost::uuids::random_generator()());
  auto const crashDirectory = _crashesDirectory / uuid;
  std::filesystem::create_directory(crashDirectory);

  DumpWriter dumpWriter(crashDirectory, _dataSourceRegistry);
  dumpWriter.dumpData(backtrace);
}

}  // namespace arangodb::crash_handler
