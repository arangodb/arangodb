////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
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
/// @author Manuel Pöter
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <filesystem>
#include <memory>
#include <string>

#include "Replication2/ReplicatedLog/LogCommon.h"

namespace arangodb::replication2::storage::wal {

struct IFileManager;

struct WalManager {
  explicit WalManager(std::string folderPath);

  auto createFileManager(LogId) -> std::unique_ptr<IFileManager>;

 private:
  void createDirectories(std::filesystem::path path);
  auto getLogPath(LogId log) const -> std::filesystem::path;
  std::filesystem::path _folderPath;
};

}  // namespace arangodb::replication2::storage::wal
