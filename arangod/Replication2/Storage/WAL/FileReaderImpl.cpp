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

#include "FileReaderImpl.h"

#include "Basics/Exceptions.h"
#include "Logger/LogMacros.h"

namespace arangodb::replication2::storage::wal {

FileReaderImpl::FileReaderImpl(std::string const& path) {
  _file = std::fopen(path.c_str(), "rb");
  if (_file == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "failed to open replicated log file");
  }
  if (setvbuf(_file, NULL, _IOFBF, 1024 * 1024) != 0) {
    LOG_DEVEL << "failed to set buffer size for log file " << path;
  }
}

FileReaderImpl::~FileReaderImpl() {
  if (_file) {
    std::fclose(_file);
  }
}

auto FileReaderImpl::read(void* buffer, std::size_t n) -> std::size_t {
  return std::fread(buffer, 1, n, _file);
}

void FileReaderImpl::seek(std::uint64_t pos) {
  TRI_ASSERT(pos <= size()) << "position " << pos << " size " << size();
  std::fseek(_file, pos, SEEK_SET);
}

auto FileReaderImpl::position() const -> std::uint64_t {
  return std::ftell(_file);
}

auto FileReaderImpl::size() const -> std::uint64_t {
  auto const pos = std::ftell(_file);
  std::fseek(_file, 0, SEEK_END);
  auto const size = std::ftell(_file);
  std::fseek(_file, pos, SEEK_SET);
  return size;
}

}  // namespace arangodb::replication2::storage::wal
