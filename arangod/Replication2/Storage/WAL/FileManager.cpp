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

#include "FileManager.h"
#include <filesystem>
#include <vector>

#include "Assertions/Assert.h"
#include "Replication2/Storage/WAL/FileReaderImpl.h"
#include "Replication2/Storage/WAL/FileWriterImpl.h"

namespace arangodb::replication2::storage::wal {

FileManager::FileManager(std::filesystem::path folderPath)
    : _folderPath(std::move(folderPath)) {}

auto FileManager::listFiles() -> std::vector<std::string> {
  std::vector<std::string> result;
  for (auto const& e : std::filesystem::directory_iterator{_folderPath}) {
    TRI_ASSERT(e.is_regular_file());
    if (e.is_regular_file()) {
      result.emplace_back(e.path().filename().string());
    }
  }
  return result;
}

auto FileManager::createReader(std::string const& filename)
    -> std::unique_ptr<IFileReader> {
  return std::make_unique<FileReaderImpl>(_folderPath / filename);
}

auto FileManager::createWriter(std::string const& filename)
    -> std::unique_ptr<IFileWriter> {
  return std::make_unique<FileWriterImpl>(_folderPath / filename);
}

void FileManager::removeAll() { std::filesystem::remove_all(_folderPath); }

}  // namespace arangodb::replication2::storage::wal
