////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Manuel Pöter
////////////////////////////////////////////////////////////////////////////////

#include "WalManager.h"

#include <filesystem>

#include "Basics/Exceptions.h"
#include "Basics/FileUtils.h"
#include "Basics/Result.h"
#include "Logger/LogMacros.h"
#include "Replication2/Storage/WAL/FileManager.h"
#include "Replication2/Storage/WAL/IFileManager.h"

namespace arangodb::replication2::storage::wal {

WalManager::WalManager(std::string folderPath)
    : _folderPath(std::move(folderPath)) {
  createDirectories(_folderPath);
}

auto WalManager::createFileManager(LogId log) -> std::unique_ptr<IFileManager> {
  auto path = getLogPath(log);
  createDirectories(path);
  return std::make_unique<FileManager>(std::move(path));
}

auto WalManager::getLogPath(LogId log) const -> std::filesystem::path {
  return _folderPath / to_string(log);
}

void WalManager::createDirectories(std::filesystem::path path) {
  try {
    std::filesystem::create_directories(path);
  } catch (std::exception& e) {
    LOG_TOPIC("0e6d9", ERR, Logger::REPLICATED_WAL)
        << "Failed to create directory "
        << path.string() + " with error " + e.what();
  }

#ifdef __linux__
  do {
    auto fd = ::open(path.c_str(), O_DIRECTORY | O_RDONLY);
    ADB_PROD_ASSERT(fd >= 0) << "failed to open directory " << path.string()
                             << " with error " << strerror(errno);
    ADB_PROD_ASSERT(fsync(fd) == 0)
        << "failed to fsync directory " << path.string() << " with error "
        << strerror(errno);
    ::close(fd);
    path = path.parent_path();
  } while (path.has_relative_path());
#endif
}

}  // namespace arangodb::replication2::storage::wal
