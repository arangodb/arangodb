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

#include "Replication2/Storage/WAL/IFileReader.h"

#include <cstdio>
#include <string>

namespace arangodb::replication2::storage::wal {

struct FileReaderImpl final : IFileReader {
  FileReaderImpl(std::string const& path);
  ~FileReaderImpl();

  auto read(void* buffer, std::size_t n) -> std::size_t override;

  void seek(std::uint64_t pos) override;

  auto position() const -> std::uint64_t override;

  auto size() const -> std::uint64_t override;

 private:
  std::FILE* _file = nullptr;
};

}  // namespace arangodb::replication2::storage::wal
