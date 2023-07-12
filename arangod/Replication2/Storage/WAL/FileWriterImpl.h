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

#pragma once

#include "Replication2/Storage/WAL/IFileWriter.h"

#include <cstdio>
#include <string_view>

namespace arangodb::replication2::storage::wal {

struct FileWriterImpl final : IFileWriter {
  FileWriterImpl(std::string path);
  ~FileWriterImpl();

  auto append(std::string_view data) -> Result override;

  void truncate(std::uint64_t size) override;

  void sync() override;

  auto getReader() const -> std::unique_ptr<IFileReader> override;

 private:
  std::string _path;
  int _file = 0;
};

}  // namespace arangodb::replication2::storage::wal
