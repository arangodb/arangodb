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

#include "Basics/Exceptions.h"
#include "Basics/FileUtils.h"
#include "Basics/Result.h"
#include "Replication2/Storage/WAL/FileWriterImpl.h"

namespace arangodb::replication2::storage::wal {

WalManager::WalManager(std::string folderPath)
    : _folderPath(std::move(folderPath)) {
  if (auto res = basics::FileUtils::createDirectory(_folderPath); !res) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL, "failed to create folder for replicated log files");
  }
}

std::unique_ptr<IFileWriter> WalManager::openLog(LogId log) {
  return std::make_unique<FileWriterImpl>(getLogPath(log));
}

Result WalManager::dropLog(LogId log) {
  auto error = basics::FileUtils::remove(getLogPath(log));
  return Result{error};
}

std::string WalManager::getLogPath(LogId log) const {
  return basics::FileUtils::buildFilename(_folderPath,
                                          std::to_string(log.id()) + ".log");
}

}  // namespace arangodb::replication2::storage::wal
